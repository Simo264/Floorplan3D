#include "spatial_hashing.hpp"

#include <glm/geometric.hpp>
#include <stdexcept>

VertexId SpatialHash::snap(glm::dvec2 p)
{
  auto cell = get_cell(p);

  // Query: search 3x3 neighbourhood for an existing close vertex
  for (auto dx = -1; dx <= 1; dx++)
  {
    for (auto dy = -1; dy <= 1; dy++)
    {
      auto it = m_grid.find(CellCoord{ cell.x + dx, cell.y + dy });
      if (it == m_grid.end())
        continue;

      // If found, return its id (reuse it)
      for (auto vertex : it->second)
      {
        auto diff = m_vertices[vertex] - p;
        if (glm::dot(diff, diff) < m_epsilon_sq)
          return vertex;
      }
    }
  }

  // If no existing vertex found => create a new one
  auto new_vertex = static_cast<VertexId>(m_vertices.size());
  m_vertices.push_back(glm::dvec2{ p });

  auto [it, inserted] = m_grid.try_emplace(cell);
  it->second.push_back(new_vertex);
  return new_vertex;
}

// VertexId SpatialHash::find_nearest(glm::dvec2 p) const
// {
//   if (m_vertices.empty())
//     throw std::runtime_error("No vertices in hash, cannot find nearest");

//   VertexId best_idx = 0;
//   auto best_dist2 = glm::length(m_vertices[0] - p);
//   for (auto i = 1u; i < m_vertices.size(); ++i)
//   {
//     auto d2 = glm::length(m_vertices[i] - p);
//     if (d2 < best_dist2)
//     {
//       best_dist2 = d2;
//       best_idx = static_cast<VertexId>(i);
//     }
//   }
//   return best_idx;
// }


VertexId SpatialHash::find_nearest(glm::dvec2 p) const
{
  if (m_vertices.empty())
    throw std::runtime_error("No vertices in hash, cannot find nearest");

  auto cell = get_cell(p);
  VertexId best_idx = static_cast<VertexId>(-1);
  f64 best_dist2 = std::numeric_limits<f64>::max();

  auto check_cell = [&](i32 cx, i32 cy) {
    auto it = m_grid.find(CellCoord{ cx, cy });
    if (it == m_grid.end()) return;

    for (auto vertex : it->second)
    {
      auto diff = m_vertices[vertex] - p;
      // Ottimizzazione: usa il prodotto scalare (distanza al quadrato)
      // per evitare la costosa sqrt di glm::length
      f64 dist2 = glm::dot(diff, diff);
      if (dist2 < best_dist2)
      {
        best_dist2 = dist2;
        best_idx = vertex;
      }
    }
  };

  // Ricerca ad anelli concentrici (Spiral Search)
  i32 r = 0;
  while (true)
  {
    // Controlla il perimetro dell'anello di raggio 'r'
    // Riga superiore e inferiore
    for (i32 dx = -r; dx <= r; ++dx)
    {
      check_cell(cell.x + dx, cell.y - r);
      if (r != 0)
        check_cell(cell.x + dx, cell.y + r); // Evita di ricontrollare la riga 0 due volte
    }
    // Colonna sinistra e destra (escludendo gli angoli già controllati)
    for (i32 dy = -r + 1; dy <= r; ++dy)
    {
      check_cell(cell.x - r, cell.y + dy);
      check_cell(cell.x + r, cell.y + dy);
    }

    // CONDIZIONE DI EARLY EXIT (Uscita Anticipata)
    // Se abbiamo già trovato un vertice, e la distanza minima teorica
    // per raggiungere l'anello corrente (r * epsilon) è maggiore o uguale
    // alla migliore distanza trovata, possiamo fermarci.
    if (best_idx != static_cast<VertexId>(-1))
    {
      f64 min_dist_to_ring = static_cast<f64>(r) * m_epsilon;
      if (min_dist_to_ring * min_dist_to_ring >= best_dist2)
      {
        break; // Nessun punto negli anelli successivi potrà essere più vicino
      }
    }

    // Salvaguardia per evitare loop infiniti nel caso in cui la griglia sia
    // stranamente desconnessa o vuota (anche se matematicamente improbabile se N>0)
    if (r > 10000) break;

    r++;
  }

  if (best_idx == static_cast<VertexId>(-1))
    throw std::runtime_error("No vertices found in hash");

  return best_idx;
}
